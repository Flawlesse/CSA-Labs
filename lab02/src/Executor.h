
#ifndef RISCV_SIM_EXECUTOR_H
#define RISCV_SIM_EXECUTOR_H

#include "Instruction.h"

class Executor
{
public:
	void Execute(InstructionPtr& instr, Word ip)
	{
        /* YOUR CODE HERE */
        std::optional<Word> aluResult;
		if (instr->_src1 && instr->_imm)
		{
			ComputeALU(instr, instr->_src1Val, (*(instr->_imm)), aluResult);
		}
		else if (instr->_src1 && instr->_src2)
		{
			ComputeALU(instr,instr->_src1Val, instr->_src2Val, aluResult);
		}

		WriteData(instr, ip, aluResult);

		CalculateJump(instr, ip);
	}

private:
    /* YOUR CODE HERE */
	void ComputeALU(InstructionPtr& instr, Word& A, Word& B, std::optional<Word>& aluResult)
	{
		/* 		ALU BLOCK 
        AluFunc::Add — А + Б.
		AluFunc::Sub — А - Б.
		AluFunc::And — побитовое А и Б.
		AluFunc::Or — побитовое А или Б.
		AluFunc::Xor — побитовое А xor Б.
		AluFunc::Slt — А < Б, где А и Б считаются числами со знаком.
		AluFunc::Sltu — А < Б, где А и Б считаются беззнаковыми.
		AluFunc::Sll — сдвиг А на В влево, где В = Б % 32.
		AluFunc::Srl — беззнаковый сдвиг А на В вправо, где В = Б % 32.
		AluFunc::Sra — знаковый сдвиг А на  В вправо, где В = Б % 32.
		AluFunc::Sr - разбивается на AluFunc::Srl и AluFunc::Sra в декодере
		AluFunc::None - ничего не делать.
		*/
		switch (instr->_aluFunc)
		{
			case AluFunc::Add:
			aluResult = A + B;
			break;

			case AluFunc::Sub:
			aluResult = A - B;
			break;

			case AluFunc::And:
			aluResult = A & B;
			break;

			case AluFunc::Or:
			aluResult = A | B;
			break;

			case AluFunc::Xor:
			aluResult = A ^ B;
			break;

			case AluFunc::Slt:
			aluResult = (Word)((SignedWord)A < (SignedWord)B);
			break;

			case AluFunc::Sltu:
			aluResult = (Word)(A < B);
			break;

			case AluFunc::Sll:
			aluResult = (A << (B % 32));
			break;

			case AluFunc::Srl:
			aluResult = (A >> (B % 32));
			break;

			case AluFunc::Sra:
			if (A & 0x80000000)
			{
				aluResult = ((B % 32) ? ((Word(0xffffffff) << (32 - (B % 32))) ^ (A >> (B % 32))) : A);
			}
			else
			{
				aluResult = (A >> (B % 32));
			}
			break;

			default: break;
		}

		if (aluResult && (instr->_type == IType::Ld || instr->_type == IType::St))
			instr->_addr = *aluResult;
	}

	void WriteData(InstructionPtr& instr, Word& ip, std::optional<Word> aluResult)
	{
		/*		LOGIC BLOCK
		IType::Csrr — записать _csrVal.
		IType::Csrw — записать _src1Val.
		IType::St — записать _src2Val.
		IType::J и IType::Jr — записать адрес текущей инструкции увеличенный на 4.
		IType::Auipc — записать адрес текущей инструкции увеличенный на _imm.
		IType::<remaining> - записать результат вычислений ALU
        */
		switch (instr->_type)
		{
			case IType::Csrr:
			instr->_data = instr->_csrVal;
			break;

			case IType::Csrw:
			instr->_data = instr->_src1Val;
			break;

			case IType::St:
			instr->_data = instr->_src2Val;
			break;

			case IType::J:
			case IType::Jr:
			instr->_data = ip + 4;
			break;

			case IType::Auipc:
			if (instr->_imm)
				instr->_data = ip + (*(instr->_imm));
			break;

			default:
			if (aluResult)
				instr->_data = *aluResult;
			break;
		}
	}

	void CalculateJump(InstructionPtr& instr, Word& ip)
	{
		/*		BRANCHING BLOCK
		BrFunc::Eq — равенство.
		BrFunc::Neq — неравенство.
		BrFunc::Lt — знаковое сравнение операндов на меньше.
		BrFunc::Ltu — беззнаковое сравнение операндов на меньше.
		BrFunc::Ge — знаковое сравнение операндов на больше или равно.
		BrFunc::Geu — беззнаковое сравнение операндов на больше или равно.
		BrFunc::AT — всегда истинно.
		BrFunc::NT — всегда ложно.
        */
        bool brResult;
        if (instr->_src1 && instr->_src2)
        {
        	switch (instr->_brFunc)
        	{
        		case BrFunc::Eq:
        		brResult = (instr->_src1Val == instr->_src2Val);
        		break;

        		case BrFunc::Neq:
        		brResult = (instr->_src1Val != instr->_src2Val);
        		break;

        		case BrFunc::Lt:
        		brResult = ((SignedWord)(instr->_src1Val) < (SignedWord)(instr->_src2Val));
        		break;

        		case BrFunc::Ltu:
        		brResult = (instr->_src1Val < instr->_src2Val);
        		break;

        		case BrFunc::Ge:
        		brResult = ((SignedWord)(instr->_src1Val) >= (SignedWord)(instr->_src2Val));
        		break;

        		case BrFunc::Geu:
        		brResult = (instr->_src1Val >= instr->_src2Val);
        		break;
        	}
		}
		if (instr->_brFunc == BrFunc::AT)
        	brResult = true;
        else if (instr->_brFunc == BrFunc::NT)
        	brResult = false;

		if (brResult)
		{
			switch (instr->_type)
			{
				case IType::Br:
				case IType::J:
				if (instr->_imm)
				{
					instr->_nextIp = ip + (*(instr->_imm));
				}
				else
				{
					instr->_nextIp = ip + 4;
				}
				break;

				case IType::Jr:
				if (instr->_imm)
				{

					instr->_nextIp = instr->_src1Val + (*(instr->_imm));
				}
				else
				{
					instr->_nextIp = ip + 4;
				}
				break;

				default:
				instr->_nextIp = ip + 4;
				break;
			}
		}
		else
		{
			instr->_nextIp = ip + 4;
		}
	}
};

#endif // RISCV_SIM_EXECUTOR_H
